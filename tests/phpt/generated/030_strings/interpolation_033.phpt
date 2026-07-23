--TEST--
string interpolation and concatenation vector 033
--FILE--
<?php
$word = 'pico33'; $number = 370;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico33:370]:pico33-370:6
