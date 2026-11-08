# web/STABLE

The commit the public installer (`webapp/installer/`) builds and offers: one line, the full SHA. Since V10 it is a
commit of the firmware line (the branch the firmware is developed on), fetched by `.github/workflows/emulator.yml`
and built from its own sources; `main` holds the site. A new version is checked first in the beta installer
(`preview/webapp/installer/`) and the emulator, and only then is its commit written here, by a PR.
