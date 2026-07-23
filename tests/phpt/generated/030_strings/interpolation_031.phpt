--TEST--
string interpolation and concatenation vector 031
--FILE--
<?php
$word = 'pico31'; $number = 348;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico31:348]:pico31-348:6
