# Security Policy

## Reporting a vulnerability

If you believe you have found a security issue in **esp_rtl_sdr**, please
**do not open a public GitHub issue** for anything that could help abuse RF
front-ends, USB stacks, or downstream products before a fix exists.

Email **hardcoreerik@gmail.com** with:

- Clear description
- Steps to reproduce (minimal PoC preferred)
- Component version (`esp_rtl_sdr_get_version_string()` or git tag)
- Host MCU / IDF version / dongle if relevant

Acknowledgment target: **72 hours**. Confirmed issues are fixed before public
disclosure when practical.

---

## Supported versions

| Version | Supported |
|---|---|
| Latest published release / tag | Yes |
| Older tags | No guarantee |

---

## Scope

### In scope

- Failures of **fail-closed** lifecycle (half-open USB, use-after-uninstall)
- Reentrancy / concurrency issues that corrupt control state
- Unsafe handling of untrusted USB descriptors from attached devices
- Privilege or isolation issues if this component is used inside a larger product
  and a clear API contract is violated by the driver itself

### Out of scope

- Bugs in ESP-IDF USB Host, FreeRTOS, or third-party BSPs
- RF regulatory compliance of end products (operator responsibility)
- Damage from high-power RF into the dongle (hardware misuse)
- Security of librtlsdr / desktop SDR apps
- Formal claims about side-channel or TEMPEST properties

---

## Honesty note

This driver is **AI-assisted, single-maintainer, early (0.x)**. There has been
**no formal third-party security audit**. Reviews are best-effort (human + AI
adversarial passes). See [docs/AI_DEVELOPMENT_DISCLOSURE.md](docs/AI_DEVELOPMENT_DISCLOSURE.md).

Responsible disclosure credit can appear in release notes unless you ask to
remain anonymous. No bug bounty at this time.
