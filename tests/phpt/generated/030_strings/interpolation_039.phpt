--TEST--
string interpolation and concatenation vector 039
--FILE--
<?php
$word = 'pico39'; $number = 436;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico39:436]:pico39-436:6
