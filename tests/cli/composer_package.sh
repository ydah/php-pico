#!/bin/sh
set -eu

repo_root=$(cd -- "$(dirname -- "$0")/../.." >/dev/null && pwd)
temporary_root=$(mktemp -d)
trap 'rm -rf "$temporary_root"' EXIT HUP INT TERM

package_root="$temporary_root/package"
consumer_root="$temporary_root/consumer"
mkdir -p "$package_root" "$consumer_root"

git -C "$repo_root" ls-files -z --cached --others --exclude-standard |
    tar --null -T - -cf - -C "$repo_root" |
    tar -xf - -C "$package_root"

cat > "$consumer_root/composer.json" <<JSON
{
    "name": "php-pico/composer-install-test",
    "version": "1.0.0",
    "repositories": [
        {
            "type": "path",
            "url": "$package_root",
            "options": {
                "symlink": false,
                "versions": {
                    "ydah/php-pico": "1.0.0"
                }
            }
        },
        {
            "packagist.org": false
        }
    ],
    "require": {
        "ydah/php-pico": "1.0.0"
    }
}
JSON

export COMPOSER_HOME="$temporary_root/composer-home"
composer --working-dir="$consumer_root" update \
    --no-interaction --no-plugins --no-scripts --prefer-dist

version=$("$consumer_root/vendor/bin/php-pico" --version)
test "$version" = "php-pico 1.0.0"

output=$("$consumer_root/vendor/bin/php-pico" -r 'echo 6 * 7, PHP_EOL;')
test "$output" = "42"

printf 'Composer package install: PASS\n'
