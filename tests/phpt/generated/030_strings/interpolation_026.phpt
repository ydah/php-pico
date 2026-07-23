--TEST--
string interpolation and concatenation vector 026
--FILE--
<?php
$word = 'pico26'; $number = 293;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico26:293]:pico26-293:6
