--TEST--
string search and slice vector 015
--FILE--
<?php
$text = 'fneedle15pppp';
echo strpos($text, 'needle15'), ':', (str_contains($text, 'needle15') ? 1 : 0), ':', (str_starts_with($text, 'f') ? 1 : 0), ':', (str_ends_with($text, 'pppp') ? 1 : 0), ':', substr($text, 1, 8);
--EXPECT--
1:1:1:1:needle15
