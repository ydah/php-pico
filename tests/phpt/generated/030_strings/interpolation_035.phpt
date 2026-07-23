--TEST--
string interpolation and concatenation vector 035
--FILE--
<?php
$word = 'pico35'; $number = 392;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico35:392]:pico35-392:6
