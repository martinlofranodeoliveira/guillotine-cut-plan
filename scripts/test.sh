#!/usr/bin/env bash
set -euo pipefail
make test
make sanitize-test
