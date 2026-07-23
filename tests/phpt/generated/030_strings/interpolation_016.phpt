--TEST--
string interpolation and concatenation vector 016
--FILE--
<?php
$word = 'pico16'; $number = 183;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico16:183]:pico16-183:6
