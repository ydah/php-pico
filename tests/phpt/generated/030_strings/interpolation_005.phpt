--TEST--
string interpolation and concatenation vector 005
--FILE--
<?php
$word = 'pico05'; $number = 62;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico05:62]:pico05-62:6
