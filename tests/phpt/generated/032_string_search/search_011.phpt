--TEST--
string search and slice vector 011
--FILE--
<?php
$text = 'bbbneedle11llll';
echo strpos($text, 'needle11'), ':', (str_contains($text, 'needle11') ? 1 : 0), ':', (str_starts_with($text, 'bbb') ? 1 : 0), ':', (str_ends_with($text, 'llll') ? 1 : 0), ':', substr($text, 3, 8);
--EXPECT--
3:1:1:1:needle11
