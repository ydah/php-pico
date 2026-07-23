--TEST--
string search and slice vector 029
--FILE--
<?php
$text = 'jjjneedle29tt';
echo strpos($text, 'needle29'), ':', (str_contains($text, 'needle29') ? 1 : 0), ':', (str_starts_with($text, 'jjj') ? 1 : 0), ':', (str_ends_with($text, 'tt') ? 1 : 0), ':', substr($text, 3, 8);
--EXPECT--
3:1:1:1:needle29
