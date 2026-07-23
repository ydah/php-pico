--TEST--
string interpolation and concatenation vector 028
--FILE--
<?php
$word = 'pico28'; $number = 315;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico28:315]:pico28-315:6
