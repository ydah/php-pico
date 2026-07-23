--TEST--
string interpolation and concatenation vector 013
--FILE--
<?php
$word = 'pico13'; $number = 150;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico13:150]:pico13-150:6
