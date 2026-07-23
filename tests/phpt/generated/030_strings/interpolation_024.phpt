--TEST--
string interpolation and concatenation vector 024
--FILE--
<?php
$word = 'pico24'; $number = 271;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico24:271]:pico24-271:6
