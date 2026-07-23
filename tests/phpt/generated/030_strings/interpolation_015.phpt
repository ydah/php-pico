--TEST--
string interpolation and concatenation vector 015
--FILE--
<?php
$word = 'pico15'; $number = 172;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico15:172]:pico15-172:6
