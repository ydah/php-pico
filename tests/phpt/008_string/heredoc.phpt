--TEST--
heredoc indentation, interpolation, nowdoc, and escapes
--FILE--
<?php
$name = 'pico';
$value = ['key' => 'ok'];
echo <<<TEXT
    hello $name {$value['key']}
    TEXT;
echo ':';
echo <<<'RAW'
    $name\n
    RAW;
echo ':', "\x41\101\u{1F600}\\q";
--EXPECT--
hello pico ok:$name\n:AA😀\q
