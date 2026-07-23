--TEST--
string interpolation and concatenation vector 020
--FILE--
<?php
$word = 'pico20'; $number = 227;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico20:227]:pico20-227:6
