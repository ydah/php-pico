--TEST--
string interpolation and concatenation vector 000
--FILE--
<?php
$word = 'pico00'; $number = 7;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico00:7]:pico00-7:6
