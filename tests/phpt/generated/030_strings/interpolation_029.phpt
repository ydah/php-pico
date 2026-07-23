--TEST--
string interpolation and concatenation vector 029
--FILE--
<?php
$word = 'pico29'; $number = 326;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico29:326]:pico29-326:6
