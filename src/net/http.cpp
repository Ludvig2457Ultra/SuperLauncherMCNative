#include "http.h"
#include "../core/common.h"
#include "../core/paths.h"
#include "../core/log.h"
#include "../core/win.h"

#if defined(SL_USE_OPENSSL)
// ------------------------------------------------------------------
// Реализация на Winsock + OpenSSL (TLS 1.2/1.3).
// Используется в XP-сборке: WinINet/Schannel на XP умеет только TLS 1.0,
// а все современные API требуют TLS 1.2+. OpenSSL 1.1.1 работает на XP.
// ------------------------------------------------------------------
#include <winsock2.h>
#include <ws2tcpip.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

namespace sl {

using std::string;

struct Url {
    bool https = false;
    string host;
    int port = 0;
    string path; // включая query
};

static bool parse_url(const string& url, Url& u) {
    string s = url;
    if (s.compare(0, 8, "https://") == 0) { u.https = true;  s = s.substr(8); }
    else if (s.compare(0, 7, "http://") == 0) { u.https = false; s = s.substr(7); }
    else return false;
    size_t slash = s.find('/');
    string hostport = slash == string::npos ? s : s.substr(0, slash);
    u.path = slash == string::npos ? "/" : s.substr(slash);
    if (u.path.empty()) u.path = "/";
    // host[:port] (без IPv6-поддержки на XP достаточно)
    size_t colon = hostport.rfind(':');
    if (colon != string::npos) {
        u.host = hostport.substr(0, colon);
        u.port = atoi(hostport.substr(colon + 1).c_str());
    } else {
        u.host = hostport;
        u.port = u.https ? 443 : 80;
    }
    return !u.host.empty();
}

static void ensure_wsa() {
    static bool init = false;
    if (init) return;
    WSADATA wd;
    if (WSAStartup(MAKEWORD(2, 2), &wd) == 0) init = true;
}

static SSL_CTX* ssl_ctx() {
    static SSL_CTX* ctx = nullptr;
    if (!ctx) {
        SSL_library_init();
        SSL_load_error_strings();
        ctx = SSL_CTX_new(TLS_client_method());
        SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
        SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
        SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION);
    }
    return ctx;
}

// Блокирующий connect с таймаутом через неблокирующий сокет + select.
static SOCKET tcp_connect(const string& host, int port, int timeout_ms) {
    ensure_wsa();
    addrinfo hints = {}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    char portstr[16];
    _snprintf(portstr, sizeof(portstr), "%d", port);
    if (getaddrinfo(host.c_str(), portstr, &hints, &res) != 0 || !res) return INVALID_SOCKET;

    SOCKET s = INVALID_SOCKET;
    for (addrinfo* ai = res; ai; ai = ai->ai_next) {
        s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s == INVALID_SOCKET) continue;
        u_long nb = 1;
        ioctlsocket(s, FIONBIO, &nb);
        int rc = connect(s, ai->ai_addr, (int)ai->ai_addrlen);
        if (rc == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) {
            fd_set wf; FD_ZERO(&wf); FD_SET(s, &wf);
            timeval tv; tv.tv_sec = timeout_ms / 1000; tv.tv_usec = (timeout_ms % 1000) * 1000;
            rc = select(0, nullptr, &wf, nullptr, &tv);
            if (rc > 0) {
                int err = 0; int el = sizeof(err);
                getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &el);
                rc = err == 0 ? 0 : SOCKET_ERROR;
            } else {
                rc = SOCKET_ERROR;
            }
        }
        u_long blk = 0;
        ioctlsocket(s, FIONBIO, &blk);
        if (rc == 0) break;
        closesocket(s);
        s = INVALID_SOCKET;
    }
    freeaddrinfo(res);
    if (s != INVALID_SOCKET) {
        // таймауты чтения/записи
        DWORD t = (DWORD)timeout_ms;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&t, sizeof(t));
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&t, sizeof(t));
    }
    return s;
}

struct HttpConn {
    SOCKET sock = INVALID_SOCKET;
    SSL* ssl = nullptr;
    bool tls = false;

    bool send_all(const char* data, size_t n) {
        size_t off = 0;
        while (off < n) {
            int w;
            if (tls) w = SSL_write(ssl, data + off, (int)(n - off));
            else     w = send(sock, data + off, (int)(n - off), 0);
            if (w <= 0) {
                if (tls) {
                    int e = SSL_get_error(ssl, w);
                    if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) { Sleep(5); continue; }
                }
                return false;
            }
            off += (size_t)w;
        }
        return true;
    }

    bool recv_some(char* buf, size_t cap, size_t& got) {
        if (tls) {
            int r = SSL_read(ssl, buf, (int)cap);
            if (r > 0) { got = (size_t)r; return true; }
            int e = SSL_get_error(ssl, r);
            if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) { got = 0; return true; }
            return false;
        }
        int r = recv(sock, buf, (int)cap, 0);
        if (r > 0) { got = (size_t)r; return true; }
        return false;
    }

    void close() {
        if (tls && ssl) { SSL_shutdown(ssl); SSL_free(ssl); }
        if (sock != INVALID_SOCKET) closesocket(sock);
        sock = INVALID_SOCKET; ssl = nullptr; tls = false;
    }
};

struct Resp {
    int status = 0;
    long long content_length = -1;
    bool chunked = false;
    string body;
};

// Читает заголовки до \r\n\r\n, затем тело (по Content-Length или chunked).
static bool read_response(HttpConn& c, Resp& r) {
    string head;
    char buf[16384];
    bool in_body = false;
    size_t content_read = 0;
    string chunk_rem;
    long long chunk_left = -1;

    for (;;) {
        size_t got = 0;
        if (!c.recv_some(buf, sizeof(buf), got)) break;
        if (got == 0) {
            if (head.empty() && r.body.empty()) { Sleep(2); continue; }
            break;
        }
        size_t pos = 0;
        while (pos < got) {
            if (!in_body) {
                size_t avail = got - pos;
                size_t need = head.empty() ? 0 : 0;
                head.append(buf + pos, avail);
                pos = got;
                size_t hdr_end = head.find("\r\n\r\n");
                if (hdr_end != string::npos) {
                    // статус
                    size_t sp = head.find(' ');
                    if (sp != string::npos) r.status = atoi(head.c_str() + sp + 1);
                    // заголовки
                    size_t l = head.find("Content-Length:", 0);
                    if (l != string::npos) {
                        l += 15;
                        while (l < head.size() && (head[l] == ' ' || head[l] == '\t')) l++;
                        r.content_length = _strtoi64(head.c_str() + l, nullptr, 10);
                    }
                    if (head.find("Transfer-Encoding: chunked") != string::npos ||
                        head.find("transfer-encoding: chunked") != string::npos) r.chunked = true;
                    size_t body_start = hdr_end + 4;
                    string leftover = head.substr(body_start);
                    head = head.substr(0, body_start);
                    in_body = true;
                    if (!leftover.empty()) {
                        buf[0] = 0;
                        memmove(buf, leftover.data(), leftover.size());
                        pos = 0;
                        got = leftover.size();
                        continue; // обработаем тело ниже
                    }
                }
            } else {
                size_t n = got - pos;
                if (r.chunked) {
                    // простой chunked-декодер
                    while (n > 0) {
                        if (chunk_left < 0) {
                            size_t nl = string(buf + pos, n).find("\r\n");
                            if (nl == string::npos) {
                                chunk_rem.append(buf + pos, n);
                                n = 0; break;
                            }
                            string szline = chunk_rem + string(buf + pos, nl);
                            chunk_rem.clear();
                            chunk_left = strtol(szline.c_str(), nullptr, 16);
                            pos += nl + 2;
                            n = got - pos;
                            if (chunk_left == 0) {
                                c.close();
                                return true;
                            }
                        } else {
                            size_t take = (size_t)((long long)n < chunk_left ? (long long)n : chunk_left);
                            r.body.append(buf + pos, take);
                            pos += take;
                            chunk_left -= (long long)take;
                            n = got - pos;
                            if (chunk_left == 0) {
                                // пропускаем \r\n
                                if (n >= 2) { pos += 2; n = got - pos; }
                                chunk_left = -1;
                            }
                        }
                    }
                } else {
                    r.body.append(buf + pos, n);
                    pos = got;
                    content_read += (long long)n;
                    if (r.content_length >= 0 && content_read >= r.content_length) {
                        c.close();
                        return true;
                    }
                }
            }
        }
    }
    if (r.content_length < 0 && !r.chunked) {
        c.close();
        return true; // читаем до закрытия
    }
    if (r.content_length >= 0 && content_read < r.content_length) {
        // не хватило — обрыв
        c.close();
        return false;
    }
    c.close();
    return true;
}

static string do_request(const char* method, const char* url, const string& body,
                         const vector<HttpHead>& headers, const NetConfig& cfg) {
    Url u;
    if (!parse_url(url, u)) return string();
    int timeout_ms = cfg.timeout_seconds * 1000;

    // Прокси: HTTP-коннект к proxy. Для https используем CONNECT-туннель.
    SOCKET s;
    bool via_proxy = !cfg.proxy_host.empty() && cfg.proxy_port > 0;
    if (via_proxy) s = tcp_connect(cfg.proxy_host, cfg.proxy_port, timeout_ms);
    else           s = tcp_connect(u.host, u.port, timeout_ms);
    if (s == INVALID_SOCKET) return string();

    HttpConn c; c.sock = s;

    if (via_proxy && u.https) {
        string req = "CONNECT " + u.host + ":" + std::to_string(u.port) + " HTTP/1.1\r\nHost: " + u.host + ":" + std::to_string(u.port) + "\r\n\r\n";
        if (!c.send_all(req.data(), req.size())) { c.close(); return string(); }
        // прочитать ответ прокси до \r\n\r\n
        string hdr;
        char b[512];
        bool ok = false;
        for (int i = 0; i < 20 && !ok; i++) {
            size_t got = 0;
            if (!c.recv_some(b, sizeof(b), got)) break;
            if (got == 0) { Sleep(10); continue; }
            hdr.append(b, got);
            size_t end = hdr.find("\r\n\r\n");
            if (end != string::npos) {
                ok = hdr.find(" 200 ") != string::npos;
                break;
            }
        }
        if (!ok) { c.close(); return string(); }
    }

    if (u.https) {
        c.tls = true;
        c.ssl = SSL_new(ssl_ctx());
        if (!c.ssl) { c.close(); return string(); }
        BIO* bio = BIO_new_socket((int)c.sock, BIO_NOCLOSE);
        SSL_set_bio(c.ssl, bio, bio);
        SSL_set_tlsext_host_name(c.ssl, u.host.c_str());
        SSL_set_connect_state(c.ssl);
        int h = 0;
        for (;;) {
            int rc = SSL_connect(c.ssl);
            if (rc == 1) break;
            int e = SSL_get_error(c.ssl, rc);
            if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) { Sleep(5); continue; }
            if (++h > 200) { c.close(); return string(); }
            c.close(); return string();
        }
    }

    // построить запрос
    string req = string(method) + " ";
    if (via_proxy && !u.https) req += string(url);
    else req += u.path;
    req += " HTTP/1.1\r\n";
    req += "Host: " + u.host;
    if ((u.https && u.port != 443) || (!u.https && u.port != 80)) req += ":" + std::to_string(u.port);
    req += "\r\n";
    req += "User-Agent: " + cfg.user_agent + "\r\n";
    for (auto& h : headers) req += h.name + ": " + h.value + "\r\n";
    if (!body.empty()) req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    req += "Connection: close\r\n\r\n";
    if (!body.empty()) req += body;

    if (!c.send_all(req.data(), req.size())) { c.close(); return string(); }

    Resp r;
    if (!read_response(c, r)) return string();
    if (r.status >= 200 && r.status < 300) return r.body;
    return string();
}

string http_request(const char* method, const char* url, const std::string& body,
                    const std::vector<HttpHead>& headers, const NetConfig& cfg) {
    return do_request(method, url, body, headers, cfg);
}

string http_get(const char* url, const NetConfig& cfg) {
    return do_request("GET", url, string(), {}, cfg);
}

string http_get_ex(const char* url, const std::vector<HttpHead>& headers,
                   const NetConfig& cfg) {
    return do_request("GET", url, string(), headers, cfg);
}

string http_post_json(const char* url, const std::string& body,
                      const std::vector<HttpHead>& headers, const NetConfig& cfg) {
    std::vector<HttpHead> h = headers;
    h.push_back({"Content-Type", "application/json"});
    return do_request("POST", url, body, h, cfg);
}

bool http_download(const char* url, const std::string& dst,
                   std::function<void(long long, long long, void*)> progress,
                   void* data, const NetConfig& cfg) {
    Url u;
    if (!parse_url(url, u)) return false;
    int timeout_ms = cfg.timeout_seconds * 1000;

    bool via_proxy = !cfg.proxy_host.empty() && cfg.proxy_port > 0;
    SOCKET s = via_proxy ? tcp_connect(cfg.proxy_host, cfg.proxy_port, timeout_ms)
                         : tcp_connect(u.host, u.port, timeout_ms);
    if (s == INVALID_SOCKET) return false;

    HttpConn c; c.sock = s;

    if (via_proxy && u.https) {
        string req = "CONNECT " + u.host + ":" + std::to_string(u.port) + " HTTP/1.1\r\nHost: " + u.host + ":" + std::to_string(u.port) + "\r\n\r\n";
        if (!c.send_all(req.data(), req.size())) { c.close(); return false; }
        string hdr; char b[512]; bool ok = false;
        for (int i = 0; i < 20 && !ok; i++) {
            size_t got = 0;
            if (!c.recv_some(b, sizeof(b), got)) break;
            if (got == 0) { Sleep(10); continue; }
            hdr.append(b, got);
            size_t end = hdr.find("\r\n\r\n");
            if (end != string::npos) { ok = hdr.find(" 200 ") != string::npos; break; }
        }
        if (!ok) { c.close(); return false; }
    }

    if (u.https) {
        c.tls = true;
        c.ssl = SSL_new(ssl_ctx());
        if (!c.ssl) { c.close(); return false; }
        BIO* bio = BIO_new_socket((int)c.sock, BIO_NOCLOSE);
        SSL_set_bio(c.ssl, bio, bio);
        SSL_set_tlsext_host_name(c.ssl, u.host.c_str());
        SSL_set_connect_state(c.ssl);
        for (;;) {
            int rc = SSL_connect(c.ssl);
            if (rc == 1) break;
            int e = SSL_get_error(c.ssl, rc);
            if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) { Sleep(5); continue; }
            c.close(); return false;
        }
    }

    string req = string("GET ") + (via_proxy && !u.https ? string(url) : u.path);
    req += " HTTP/1.1\r\nHost: " + u.host;
    if ((u.https && u.port != 443) || (!u.https && u.port != 80)) req += ":" + std::to_string(u.port);
    req += "\r\nUser-Agent: " + cfg.user_agent + "\r\nConnection: close\r\n\r\n";
    if (!c.send_all(req.data(), req.size())) { c.close(); return false; }

    Resp r;
    if (!read_response(c, r)) return false;
    if (r.status < 200 || r.status >= 300) return false;

    if (!mkdirs(parent_dir(dst))) {
        log_error("http_download: cannot create dir for " + dst);
    }
    string tmp = dst + ".part";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) { c.close(); log_error("http_download: cannot open " + tmp); return false; }

    size_t written = fwrite(r.body.data(), 1, r.body.size(), f);
    if (progress) progress((long long)written, r.content_length, data);
    if (written != r.body.size()) {
        fclose(f); remove_file(tmp); c.close(); return false;
    }
    fclose(f);

    remove_file(dst);
    if (!MoveFileA(tmp.c_str(), dst.c_str())) {
        if (!copy_file(tmp, dst)) { remove_file(tmp); c.close(); return false; }
        remove_file(tmp);
    }
    c.close();
    return true;
}

} // namespace sl

#else // !SL_USE_OPENSSL
// ------------------------------------------------------------------
// Реализация на WinINet (используется в MSVC-сборке).
// ------------------------------------------------------------------
#include <wininet.h>
#include <cstdio>

#pragma comment(lib, "wininet.lib")

namespace sl {

using std::string;

static INTERNET_PROXY_INFO make_proxy(const NetConfig& cfg, std::wstring& scratch) {
    INTERNET_PROXY_INFO pi = {0};
    std::wstring u = a2w(cfg.proxy_user), p = a2w(cfg.proxy_pass);
    scratch = L"http://" + u;
    if (!cfg.proxy_user.empty() && !cfg.proxy_pass.empty()) scratch += L":" + p;
    scratch += L"@" + a2w(cfg.proxy_host) + L":" + std::to_wstring(cfg.proxy_port);
    pi.dwAccessType = INTERNET_OPEN_TYPE_PROXY;
    pi.lpszProxy = scratch.c_str();
    pi.lpszProxyBypass = L"<local>";
    return pi;
}

static HINTERNET open_session(const NetConfig& cfg) {
    if (!cfg.proxy_host.empty() && cfg.proxy_port > 0) {
        return InternetOpenA(cfg.user_agent.c_str(),
                             INTERNET_OPEN_TYPE_PRECONFIG_WITH_NO_AUTOPROXY,
                             nullptr, nullptr, 0);
    }
    return InternetOpenA(cfg.user_agent.c_str(), INTERNET_OPEN_TYPE_PRECONFIG,
                         nullptr, nullptr, 0);
}

string http_request(const char* method, const char* url, const std::string& body,
                    const std::vector<HttpHead>& headers, const NetConfig& cfg) {
    HINTERNET hNet = open_session(cfg);
    if (!hNet) return string();
    if (!cfg.proxy_host.empty() && cfg.proxy_port > 0) {
        std::wstring scratch;
        INTERNET_PROXY_INFO pi = make_proxy(cfg, scratch);
        InternetSetOptionW(hNet, INTERNET_OPTION_PROXY, &pi, sizeof(pi));
    }

    string hdr;
    for (auto& h : headers) hdr += h.name + ": " + h.value + "\r\n";

    HINTERNET hUrl = InternetOpenUrlA(hNet, url, hdr.empty() ? nullptr : hdr.c_str(),
                                      (DWORD)hdr.size(),
                                      INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_COOKIES, 0);
    if (!hUrl) { InternetCloseHandle(hNet); return string(); }

    if (_stricmp(method, "POST") == 0 || _stricmp(method, "PUT") == 0) {
        DWORD sent = 0;
        InternetWriteFile(hUrl, body.data(), (DWORD)body.size(), &sent);
    }

    string out;
    char buf[16384];
    DWORD rd;
    while (InternetReadFile(hUrl, buf, sizeof(buf), &rd) && rd > 0) {
        out.append(buf, rd);
    }
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hNet);
    return out;
}

string http_get(const char* url, const NetConfig& cfg) {
    return http_request("GET", url, string(), {}, cfg);
}

string http_get_ex(const char* url, const std::vector<HttpHead>& headers,
                   const NetConfig& cfg) {
    return http_request("GET", url, string(), headers, cfg);
}

string http_post_json(const char* url, const std::string& body,
                      const std::vector<HttpHead>& headers, const NetConfig& cfg) {
    std::vector<HttpHead> h = headers;
    h.push_back({"Content-Type", "application/json"});
    return http_request("POST", url, body, h, cfg);
}

bool http_download(const char* url, const std::string& dst,
                   std::function<void(long long, long long, void*)> progress,
                   void* data, const NetConfig& cfg) {
    HINTERNET hNet = open_session(cfg);
    if (!hNet) return false;
    if (!cfg.proxy_host.empty() && cfg.proxy_port > 0) {
        std::wstring scratch;
        INTERNET_PROXY_INFO pi = make_proxy(cfg, scratch);
        InternetSetOptionW(hNet, INTERNET_OPTION_PROXY, &pi, sizeof(pi));
    }
    HINTERNET hUrl = InternetOpenUrlA(hNet, url, nullptr, 0,
                                      INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_COOKIES, 0);
    if (!hUrl) { InternetCloseHandle(hNet); return false; }

    long long total = -1;
    {
        char lenbuf[32]; DWORD len = sizeof(lenbuf); DWORD idx = 0;
        if (HttpQueryInfoA(hUrl, HTTP_QUERY_CONTENT_LENGTH, lenbuf, &len, &idx)) {
            total = _strtoi64(lenbuf, nullptr, 10);
        }
    }

    if (!mkdirs(parent_dir(dst))) {
        log_error("http_download: cannot create dir for " + dst);
    }

    string tmp = dst + ".part";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) { InternetCloseHandle(hUrl); InternetCloseHandle(hNet); return false; }

    char buf[65536];
    DWORD rd;
    long long got = 0;
    bool ok = true;
    while (InternetReadFile(hUrl, buf, sizeof(buf), &rd)) {
        if (rd == 0) break;
        fwrite(buf, 1, rd, f);
        got += rd;
        if (progress) progress(got, total, data);
    }
    if (GetLastError() != ERROR_SUCCESS && got == 0 && total > 0) ok = false;
    fclose(f);

    if (!ok) {
        remove_file(tmp);
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hNet);
        return false;
    }
    remove_file(dst);
    if (!MoveFileA(tmp.c_str(), dst.c_str())) {
        if (!copy_file(tmp, dst)) ok = false;
        remove_file(tmp);
    }

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hNet);
    if (!ok) log_error("http_download failed: " + string(url));
    return ok;
}

} // namespace sl

#endif // SL_USE_OPENSSL
