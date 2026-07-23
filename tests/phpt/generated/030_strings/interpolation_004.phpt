--TEST--
string interpolation and concatenation vector 004
--FILE--
<?php
$word = 'pico04'; $number = 51;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico04:51]:pico04-51:6
