--TEST--
string interpolation and concatenation vector 037
--FILE--
<?php
$word = 'pico37'; $number = 414;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico37:414]:pico37-414:6
