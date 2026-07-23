--TEST--
string interpolation and concatenation vector 018
--FILE--
<?php
$word = 'pico18'; $number = 205;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico18:205]:pico18-205:6
