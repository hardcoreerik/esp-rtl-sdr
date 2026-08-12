# Licensing

**esp_rtl_sdr** uses the same dual-license model as related Orc projects
([TheOrc](https://github.com/hardcoreerik/TheOrc), [OrcSDR](https://github.com/hardcoreerik/OrcSDR)):

1. **AGPL-3.0-only** (default for the public tree) — see [LICENSE](LICENSE).
2. **Commercial** terms available by agreement — contact hardcoreerik@gmail.com.

## What the open-source license covers

- All first-party source in this repository (including AI-assisted contributions —
  see [docs/AI_DEVELOPMENT_DISCLOSURE.md](docs/AI_DEVELOPMENT_DISCLOSURE.md)).
- Documentation and examples in this tree.

## What this is not

- **Not** a redistributed copy of librtlsdr / rtl-sdr-blog / osmocom rtl-sdr.
  Transfer tables are clean-room / measured. See [docs/CLEAN_ROOM.md](docs/CLEAN_ROOM.md).
- **Not** a grant of rights to Realtek, Rafael Micro, or RTL-SDR Blog trademarks
  or confidential datasheets. Public product docs and community-available
  register material are cited for *insight* only ([docs/SILICON.md](docs/SILICON.md)).

## Commercial

If you need this driver inside a proprietary product without AGPL obligations,
ask about commercial licensing rather than re-implementing against clean-room
tables.

## Independence

This repository is **independent** of the OrcSDR application monorepo. OrcSDR
may consume this component as a dependency; that does not merge the projects or
their release trains.
