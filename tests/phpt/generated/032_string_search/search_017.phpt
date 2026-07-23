--TEST--
string search and slice vector 017
--FILE--
<?php
$text = 'hhhneedle17rr';
echo strpos($text, 'needle17'), ':', (str_contains($text, 'needle17') ? 1 : 0), ':', (str_starts_with($text, 'hhh') ? 1 : 0), ':', (str_ends_with($text, 'rr') ? 1 : 0), ':', substr($text, 3, 8);
--EXPECT--
3:1:1:1:needle17
