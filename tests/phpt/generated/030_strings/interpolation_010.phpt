--TEST--
string interpolation and concatenation vector 010
--FILE--
<?php
$word = 'pico10'; $number = 117;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico10:117]:pico10-117:6
