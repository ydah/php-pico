--TEST--
string interpolation and concatenation vector 009
--FILE--
<?php
$word = 'pico09'; $number = 106;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico09:106]:pico09-106:6
