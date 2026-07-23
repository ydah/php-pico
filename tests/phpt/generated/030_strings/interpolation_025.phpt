--TEST--
string interpolation and concatenation vector 025
--FILE--
<?php
$word = 'pico25'; $number = 282;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico25:282]:pico25-282:6
