--TEST--
string interpolation and concatenation vector 002
--FILE--
<?php
$word = 'pico02'; $number = 29;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico02:29]:pico02-29:6
