--TEST--
string interpolation and concatenation vector 008
--FILE--
<?php
$word = 'pico08'; $number = 95;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico08:95]:pico08-95:6
