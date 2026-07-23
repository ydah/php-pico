--TEST--
string interpolation and concatenation vector 022
--FILE--
<?php
$word = 'pico22'; $number = 249;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico22:249]:pico22-249:6
