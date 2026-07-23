--TEST--
string interpolation and concatenation vector 012
--FILE--
<?php
$word = 'pico12'; $number = 139;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico12:139]:pico12-139:6
