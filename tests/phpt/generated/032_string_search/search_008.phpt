--TEST--
string search and slice vector 008
--FILE--
<?php
$text = 'iiineedle08s';
echo strpos($text, 'needle08'), ':', (str_contains($text, 'needle08') ? 1 : 0), ':', (str_starts_with($text, 'iii') ? 1 : 0), ':', (str_ends_with($text, 's') ? 1 : 0), ':', substr($text, 3, 8);
--EXPECT--
3:1:1:1:needle08
