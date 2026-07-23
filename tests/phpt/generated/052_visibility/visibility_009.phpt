--TEST--
private and protected member access vector 009
--FILE--
<?php
class GeneratedVisibility009 {
    private $left = 67; protected $right = 51;
    private function left() { return $this->left; }
    protected function right() { return $this->right; }
    public function total() { return $this->left() + $this->right(); }
}
echo (new GeneratedVisibility009())->total();
--EXPECT--
118
