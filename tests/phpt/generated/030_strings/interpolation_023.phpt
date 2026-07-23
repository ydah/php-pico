--TEST--
string interpolation and concatenation vector 023
--FILE--
<?php
$word = 'pico23'; $number = 260;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico23:260]:pico23-260:6
