--TEST--
string interpolation and concatenation vector 007
--FILE--
<?php
$word = 'pico07'; $number = 84;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico07:84]:pico07-84:6
