set -euo pipefail
IFS=$'\t\n'

temp=.temp
mkdir $temp
cp $ASSET_PATH $temp/$ASSET_NAME
(
	cd $temp
	# 先创建 Release（直接发布）
gh release create $GITHUB_REF_NAME $ASSET_NAME \
    --title "$RELEASE_NAME" \
    --notes "自动发布的版本" \
    --clobber \
    --draft=false \
    --prerelease=false

)
rm -r $temp
echo browser_download_url=https://github.com/$GITHUB_REPOSITORY/releases/download/$GITHUB_REF_NAME/$ASSET_NAME >> $GITHUB_OUTPUT
