--TEST--
string interpolation and concatenation vector 017
--FILE--
<?php
$word = 'pico17'; $number = 194;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico17:194]:pico17-194:6
