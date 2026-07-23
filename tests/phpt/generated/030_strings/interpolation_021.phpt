--TEST--
string interpolation and concatenation vector 021
--FILE--
<?php
$word = 'pico21'; $number = 238;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico21:238]:pico21-238:6
