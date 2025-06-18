set -euo pipefail
IFS=$'\t\n'

gh release create --verify-tag --title $RELEASE_NAME $GITHUB_REF_NAME
