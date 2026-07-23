--TEST--
string search and slice vector 004
--FILE--
<?php
$text = 'eeneedle04o';
echo strpos($text, 'needle04'), ':', (str_contains($text, 'needle04') ? 1 : 0), ':', (str_starts_with($text, 'ee') ? 1 : 0), ':', (str_ends_with($text, 'o') ? 1 : 0), ':', substr($text, 2, 8);
--EXPECT--
2:1:1:1:needle04
