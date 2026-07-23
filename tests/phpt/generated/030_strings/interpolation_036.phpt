--TEST--
string interpolation and concatenation vector 036
--FILE--
<?php
$word = 'pico36'; $number = 403;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico36:403]:pico36-403:6
