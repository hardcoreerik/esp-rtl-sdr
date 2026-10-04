#pragma once

/*
 * Function-test report core. Pure C++ (no ESP-IDF, no USB) so the report
 * format and helpers are unit-tested on the host (tests/host/test_function_test.cpp).
 *
 * Output is one JSON object per line so a PC runner can parse a serial log:
 *   {"ft":"begin","version":"..","sha":"..","board":".."}
 *   {"ft":"result","test":"..","dev":0,"profile":"..","status":"PASS","ms":12,"detail":".."}
 *   {"ft":"summary","pass":N,"fail":N,"skip":N,"devices":N,"ok":true}
 * dev is -1 for tests that are not tied to one dongle.
 */

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace ft {

enum class Status : uint8_t { Pass, Fail, Skip };

inline const char *status_name(Status s)
{
    switch (s) {
    case Status::Pass: return "PASS";
    case Status::Fail: return "FAIL";
    default: return "SKIP";
    }
}

/** JSON string escape into dst (always NUL-terminated when cap > 0). Returns length. */
inline size_t json_escape(char *dst, size_t cap, const char *src)
{
    if (dst == nullptr || cap == 0) {
        return 0;
    }
    size_t n = 0;
    for (const char *p = src != nullptr ? src : ""; *p != '\0'; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        char tmp[8];
        size_t len = 0;
        if (c == '"' || c == '\\') {
            tmp[0] = '\\';
            tmp[1] = static_cast<char>(c);
            len = 2;
        } else if (c == '\n') {
            tmp[0] = '\\'; tmp[1] = 'n'; len = 2;
        } else if (c == '\r') {
            tmp[0] = '\\'; tmp[1] = 'r'; len = 2;
        } else if (c == '\t') {
            tmp[0] = '\\'; tmp[1] = 't'; len = 2;
        } else if (c < 0x20 || c >= 0x7f) {
            tmp[0] = '?'; len = 1; /* keep the log 7-bit clean */
        } else {
            tmp[0] = static_cast<char>(c);
            len = 1;
        }
        if (n + len >= cap) {
            break;
        }
        for (size_t i = 0; i < len; ++i) {
            dst[n++] = tmp[i];
        }
    }
    dst[n] = '\0';
    return n;
}

/** |actual - expected| <= pct% of expected. expected == 0 requires actual == 0. */
inline bool within_pct(uint64_t actual, uint64_t expected, uint32_t pct)
{
    if (expected == 0) {
        return actual == 0;
    }
    const uint64_t diff = actual > expected ? actual - expected : expected - actual;
    return diff * 100u <= expected * pct;
}

using Sink = void (*)(const char *line, void *ctx);

class Reporter {
public:
    Reporter(Sink sink, void *ctx) : sink_(sink), ctx_(ctx) {}

    void begin(const char *version, const char *sha, const char *board)
    {
        char v[48], s[48], b[48], line[256];
        json_escape(v, sizeof(v), version);
        json_escape(s, sizeof(s), sha);
        json_escape(b, sizeof(b), board);
        std::snprintf(line, sizeof(line),
                      "{\"ft\":\"begin\",\"version\":\"%s\",\"sha\":\"%s\",\"board\":\"%s\"}", v, s,
                      b);
        emit(line);
    }

    __attribute__((format(printf, 7, 8)))
    void result(const char *test, int dev, const char *profile, Status st, uint32_t ms,
                const char *fmt, ...)
    {
        char raw[200];
        va_list ap;
        va_start(ap, fmt);
        std::vsnprintf(raw, sizeof(raw), fmt != nullptr ? fmt : "", ap);
        va_end(ap);

        char t[64], p[40], d[260], line[480];
        json_escape(t, sizeof(t), test);
        json_escape(p, sizeof(p), profile != nullptr ? profile : "");
        json_escape(d, sizeof(d), raw);
        std::snprintf(line, sizeof(line),
                      "{\"ft\":\"result\",\"test\":\"%s\",\"dev\":%d,\"profile\":\"%s\","
                      "\"status\":\"%s\",\"ms\":%u,\"detail\":\"%s\"}",
                      t, dev, p, status_name(st), static_cast<unsigned>(ms), d);
        switch (st) {
        case Status::Pass: ++pass_; break;
        case Status::Fail: ++fail_; break;
        default: ++skip_; break;
        }
        emit(line);
    }

    /** "ok" means nothing failed and at least one test passed. */
    bool ok() const { return fail_ == 0 && pass_ > 0; }

    void summary(int devices)
    {
        char line[160];
        std::snprintf(line, sizeof(line),
                      "{\"ft\":\"summary\",\"pass\":%u,\"fail\":%u,\"skip\":%u,\"devices\":%d,"
                      "\"ok\":%s}",
                      static_cast<unsigned>(pass_), static_cast<unsigned>(fail_),
                      static_cast<unsigned>(skip_), devices, ok() ? "true" : "false");
        emit(line);
    }

    uint32_t pass() const { return pass_; }
    uint32_t fail() const { return fail_; }
    uint32_t skip() const { return skip_; }

private:
    void emit(const char *line)
    {
        if (sink_ != nullptr) {
            sink_(line, ctx_);
        }
    }

    Sink sink_;
    void *ctx_;
    uint32_t pass_ = 0, fail_ = 0, skip_ = 0;
};

/**
 * One test case with several checks. Emits exactly one result: FAIL with the
 * first failed check (and a count) or PASS with the note, never both. A case
 * that is destroyed without finishing reports FAIL, so a forgotten finish()
 * cannot produce a silent pass.
 */
class Case {
public:
    Case(Reporter &r, const char *name, int dev, const char *profile, uint32_t (*now_ms)())
        : r_(r), name_(name), dev_(dev), profile_(profile), now_ms_(now_ms),
          t0_(now_ms != nullptr ? now_ms() : 0)
    {
    }
    Case(const Case &) = delete;
    Case &operator=(const Case &) = delete;

    ~Case()
    {
        if (!done_) {
            r_.result(name_, dev_, profile_, Status::Fail, elapsed(), "test ended without a verdict");
        }
    }

    __attribute__((format(printf, 3, 4)))
    bool check(bool cond, const char *fmt, ...)
    {
        if (cond) {
            return true;
        }
        if (fails_++ == 0) {
            va_list ap;
            va_start(ap, fmt);
            std::vsnprintf(first_, sizeof(first_), fmt, ap);
            va_end(ap);
        }
        return false;
    }

    __attribute__((format(printf, 2, 3)))
    void note(const char *fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        std::vsnprintf(note_, sizeof(note_), fmt, ap);
        va_end(ap);
    }

    __attribute__((format(printf, 2, 3)))
    void skip(const char *fmt, ...)
    {
        char why[160];
        va_list ap;
        va_start(ap, fmt);
        std::vsnprintf(why, sizeof(why), fmt, ap);
        va_end(ap);
        done_ = true;
        r_.result(name_, dev_, profile_, Status::Skip, elapsed(), "%s", why);
    }

    bool failed() const { return fails_ != 0; }

    void finish()
    {
        if (done_) {
            return;
        }
        done_ = true;
        if (fails_ == 0) {
            r_.result(name_, dev_, profile_, Status::Pass, elapsed(), "%s", note_);
        } else if (fails_ == 1) {
            r_.result(name_, dev_, profile_, Status::Fail, elapsed(), "%s", first_);
        } else {
            r_.result(name_, dev_, profile_, Status::Fail, elapsed(), "%s (+%u more)", first_,
                      static_cast<unsigned>(fails_ - 1));
        }
    }

private:
    uint32_t elapsed() const { return now_ms_ != nullptr ? now_ms_() - t0_ : 0; }

    Reporter &r_;
    const char *name_;
    int dev_;
    const char *profile_;
    uint32_t (*now_ms_)();
    uint32_t t0_;
    bool done_ = false;
    uint32_t fails_ = 0;
    char first_[160] = "";
    char note_[160] = "";
};

} // namespace ft
