--TEST--
string interpolation and concatenation vector 027
--FILE--
<?php
$word = 'pico27'; $number = 304;
echo "[$word:$number]", ':', $word . '-' . $number, ':', strlen($word);
--EXPECT--
[pico27:304]:pico27-304:6
