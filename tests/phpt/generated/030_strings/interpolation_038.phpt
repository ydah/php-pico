--TEST--
string interpolation and concatenation vector 038
--FILE--
<?php
$word = 'pico38'; $number = 425;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico38:425]:pico38-425:6
