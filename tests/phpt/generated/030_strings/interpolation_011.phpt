--TEST--
string interpolation and concatenation vector 011
--FILE--
<?php
$word = 'pico11'; $number = 128;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico11:128]:pico11-128:6
