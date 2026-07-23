--TEST--
string interpolation and concatenation vector 006
--FILE--
<?php
$word = 'pico06'; $number = 73;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico06:73]:pico06-73:6
