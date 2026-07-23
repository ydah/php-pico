--TEST--
string interpolation and concatenation vector 014
--FILE--
<?php
$word = 'pico14'; $number = 161;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico14:161]:pico14-161:6
