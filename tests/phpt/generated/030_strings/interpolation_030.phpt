--TEST--
string interpolation and concatenation vector 030
--FILE--
<?php
$word = 'pico30'; $number = 337;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico30:337]:pico30-337:6
