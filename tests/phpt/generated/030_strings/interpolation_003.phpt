--TEST--
string interpolation and concatenation vector 003
--FILE--
<?php
$word = 'pico03'; $number = 40;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico03:40]:pico03-40:6
