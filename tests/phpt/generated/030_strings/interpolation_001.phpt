--TEST--
string interpolation and concatenation vector 001
--FILE--
<?php
$word = 'pico01'; $number = 18;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico01:18]:pico01-18:6
