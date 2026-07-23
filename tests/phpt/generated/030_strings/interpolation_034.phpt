--TEST--
string interpolation and concatenation vector 034
--FILE--
<?php
$word = 'pico34'; $number = 381;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico34:381]:pico34-381:6
