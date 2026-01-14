#!/usr/bin/env bash
set -euo pipefail

# Required env:
#   GITHUB_TOKEN
#   ASSET_PATH   (path to file)
#   ASSET_NAME   (name shown in release)
#
# Optional env:
#   RELEASE_NAME (tag or release name; auto-detected if empty)

if [[ -z "${GITHUB_TOKEN:-}" ]]; then
  echo "ERROR: GITHUB_TOKEN is not set"
  exit 1
fi

if [[ -z "${ASSET_PATH:-}" ]]; then
  echo "ERROR: ASSET_PATH is not set"
  exit 1
fi

if [[ ! -f "${ASSET_PATH}" ]]; then
  echo "ERROR: ASSET_PATH does not exist: ${ASSET_PATH}"
  exit 1
fi

if [[ -z "${ASSET_NAME:-}" ]]; then
  ASSET_NAME="$(basename "${ASSET_PATH}")"
fi

# Auto-detect release name (tag)
if [[ -z "${RELEASE_NAME:-}" ]]; then
  RELEASE_NAME="$(gh release view --json tagName -q .tagName)"
fi

echo "Uploading asset:"
echo "  Release : ${RELEASE_NAME}"
echo "  File    : ${ASSET_PATH}"
echo "  Name    : ${ASSET_NAME}"

# Upload with overwrite
gh release upload "${RELEASE_NAME}" "${ASSET_PATH}" \
  --name "${ASSET_NAME}" \
  --clobber
