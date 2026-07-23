--TEST--
string interpolation and concatenation vector 019
--FILE--
<?php
$word = 'pico19'; $number = 216;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico19:216]:pico19-216:6
