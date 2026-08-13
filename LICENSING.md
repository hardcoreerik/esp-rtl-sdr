# Licensing

**esp_rtl_sdr** uses the same dual-license model as related Orc projects
([TheOrc](https://github.com/hardcoreerik/TheOrc), [OrcSDR](https://github.com/hardcoreerik/OrcSDR)):

1. **AGPL-3.0-only** (default for the public tree) — see [LICENSE](LICENSE).
2. **Commercial** license available by agreement — contact below.

---

## What the open-source license covers

- All first-party source in this repository (including AI-assisted contributions —
  see [docs/AI_DEVELOPMENT_DISCLOSURE.md](docs/AI_DEVELOPMENT_DISCLOSURE.md)).
- Documentation and examples in this tree.

AGPL means: if you distribute a modified version, or often if you provide network
access to a modified version as a service, you must offer corresponding source under
AGPL. Read the full LICENSE; this summary is not legal advice.

---

## What this is not

- **Not** a redistributed copy of librtlsdr / rtl-sdr-blog / osmocom rtl-sdr.
  Transfer tables are clean-room / measured. See [docs/CLEAN_ROOM.md](docs/CLEAN_ROOM.md).
- **Not** a grant of rights to Realtek, Rafael Micro, or RTL-SDR Blog trademarks
  or confidential datasheets. Public product docs and community-available
  register material are cited for *insight* only ([docs/SILICON.md](docs/SILICON.md)).
- **Not** MIT/BSD dual-open by default. Commercial is a **separate** paid/terms track.

---

## Commercial licensing (how to start)

If you need this driver inside a **proprietary** product without AGPL obligations
(closed firmware, appliance, SaaS that would otherwise trigger AGPL concerns):

### 1. Contact

Email: **hardcoreerik@gmail.com**  
Subject: `esp_rtl_sdr commercial license`

### 2. Include in the first message

| Item | Why |
|---|---|
| Company / product name | Scope |
| Approx. volume / SKUs | Pricing shape |
| Target ESP-IDF version & MCU | Support boundary |
| Whether you need support SLA | Optional paid support |
| Git tag you want frozen | e.g. `v0.7.3` |

### 3. What you typically get

- Non-AGPL license grant for the **esp_rtl_sdr** first-party tree (exact terms in agreement)
- Permission to ship in closed products under those terms
- Optional: priority bugfix window for the licensed tag

### 4. What you do **not** automatically get

- Rights to third-party trademarks or NDA silicon docs  
- Warranty that 0.x is production-ready  
- Support for unmeasured dongles or unclaimed MCUs  
- Librtlsdr ABI compatibility  

### 5. Process

1. Email with the table above.  
2. We reply with questions / draft terms.  
3. Signed agreement + invoice (if applicable).  
4. You pin the agreed tag/commit in your product BOM.

Do **not** re-implement against clean-room tables solely to dodge AGPL without legal review — talk first.

---

## Independence

This repository is **independent** of the OrcSDR application monorepo. OrcSDR
may consume this component as a dependency; that does not merge the projects or
their release trains.

---

## FAQ

**Q: Can I use AGPL in a hobby closed binary I never distribute?**  
A: Generally private use is fine; distribution and many network deployments are not. Read AGPL or ask a lawyer.

**Q: Is there a free MIT carve-out for non-commercial?**  
A: **No** separate MIT public track today. Non-commercial open use is AGPL; commercial closed use needs the commercial license.

**Q: Contributions?**  
A: Accepted under the same AGPL + commercial dual-license model. See [CONTRIBUTING.md](CONTRIBUTING.md).
