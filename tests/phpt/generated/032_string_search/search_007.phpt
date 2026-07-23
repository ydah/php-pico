--TEST--
string search and slice vector 007
--FILE--
<?php
$text = 'hhneedle07rrrr';
echo strpos($text, 'needle07'), ':', (str_contains($text, 'needle07') ? 1 : 0), ':', (str_starts_with($text, 'hh') ? 1 : 0), ':', (str_ends_with($text, 'rrrr') ? 1 : 0), ':', substr($text, 2, 8);
--EXPECT--
2:1:1:1:needle07
