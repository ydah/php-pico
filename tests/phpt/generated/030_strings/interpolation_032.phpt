--TEST--
string interpolation and concatenation vector 032
--FILE--
<?php
$word = 'pico32'; $number = 359;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico32:359]:pico32-359:6
